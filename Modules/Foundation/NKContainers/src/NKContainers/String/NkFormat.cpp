// =============================================================================
// @File    NkFormat.cpp
// @Brief   Implémentation du moteur de formatage positionnel {i:props}
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @Date    2026-03-06
// @License Apache-2.0
//
// @Algorithme global
//
//  NkFormatImpl() parcourt la chaîne de format :
//    - Texte littéral → appendé directement
//    - '{{'  → littéral '{'
//    - '}}'  → littéral '}'
//    - '{i}' ou '{i:props}' →
//        1. Parse l'index i
//        2. Parse les props (NkParseFormatProps)
//        3. Appelle args[i].convert(data, props) → NkString
//        4. Appende au résultat
//
//  NkParseFormatProps() :
//    Découpe "props" en tokens (espaces ou virgules).
//    Chaque token est reconnu par valeur exacte ou préfixe.
//
//  NkIntToString / NkUIntToString / NkFloatToString :
//    snprintf sur buffers stack → zéro allocation STL.
// =============================================================================

#include "pch.h"
#include "NKContainers/String/NkFormat.h"
#include "NKContainers/String/NkFormatf.h"  // NkToBinaryString

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace nkentseu {
    

        // =====================================================================
        //  Utilitaires internes
        // =====================================================================

        static bool StrEq(const char* a, const char* b) {
            while (*a && *b) { if (*a != *b) return false; ++a; ++b; }
            return *a == *b;
        }

        static bool StrStartsWith(const char* str, const char* prefix) {
            while (*prefix) {
                if (*str != *prefix) return false;
                ++str; ++prefix;
            }
            return true;
        }

        static int ParseInt(const char** p) {
            int v = 0;
            while (**p >= '0' && **p <= '9') { v = v * 10 + (**p - '0'); ++(*p); }
            return v;
        }

        // =====================================================================
        //  NkParseFormatProps
        // =====================================================================

        NkFormatProps NkParseFormatProps(const char* props, int propsLen) {
            NkFormatProps out;
            if (!props || propsLen <= 0) return out;

            char buf[128];
            int len = propsLen < 127 ? propsLen : 127;
            ::memcpy(buf, props, static_cast<size_t>(len));
            buf[len] = '\0';

            const char* p = buf;

            // Saute séparateurs
            auto skipSep = [](const char* s) -> const char* {
                while (*s == ' ' || *s == ',') ++s;
                return s;
            };

            // Lit un token jusqu'au séparateur ou '\0'
            auto readToken = [](const char* s, char* tok, int maxTok) -> const char* {
                int i = 0;
                while (*s && *s != ' ' && *s != ',') {
                    if (i < maxTok - 1) tok[i++] = *s;
                    ++s;
                }
                tok[i] = '\0';
                return s;
            };

            char tok[32];

            while (*(p = skipSep(p))) {
                p = readToken(p, tok, sizeof(tok));
                if (!tok[0]) break;

                // Alignement
                if (StrEq(tok, "<")) { out.align = NkFormatAlign::Left;   continue; }
                if (StrEq(tok, ">")) { out.align = NkFormatAlign::Right;  continue; }
                if (StrEq(tok, "^")) { out.align = NkFormatAlign::Center; continue; }

                // Flags
                if (StrEq(tok, "+")) { out.showSign   = true; continue; }
                if (StrEq(tok, "#")) { out.showPrefix = true; continue; }
                if (StrEq(tok, "0")) {
                    out.fill  = '0';
                    out.align = NkFormatAlign::Right;
                    continue;
                }

                // Bases
                if (StrEq(tok, "hex"))  { out.base = NkFormatBase::Hex;      continue; }
                if (StrEq(tok, "HEX"))  { out.base = NkFormatBase::HexUpper; continue; }
                if (StrEq(tok, "bin"))  { out.base = NkFormatBase::Bin;      continue; }
                if (StrEq(tok, "oct"))  { out.base = NkFormatBase::Oct;      continue; }

                // Style flottant
                if (StrEq(tok, "sci"))  { out.floatStyle = NkFormatFloat::Scientific; continue; }
                if (StrEq(tok, "SCI"))  { out.floatStyle = NkFormatFloat::SciUpper;   continue; }
                if (StrEq(tok, "gen"))  { out.floatStyle = NkFormatFloat::General;    continue; }

                // Casse string
                if (StrEq(tok, "upper")) { out.upperStr = true; continue; }
                if (StrEq(tok, "lower")) { out.lowerStr = true; continue; }

                // w=N
                if (StrStartsWith(tok, "w=")) {
                    const char* n = tok + 2;
                    out.width = ParseInt(&n);
                    continue;
                }

                // base=N
                if (StrStartsWith(tok, "base=")) {
                    const char* n = tok + 5;
                    int b = ParseInt(&n);
                    switch (b) {
                        case 2:  out.base = NkFormatBase::Bin; break;
                        case 8:  out.base = NkFormatBase::Oct; break;
                        case 16: out.base = NkFormatBase::Hex; break;
                        default: out.base = NkFormatBase::Dec; break;
                    }
                    continue;
                }

                // fill=C
                if (StrStartsWith(tok, "fill=") && tok[5]) {
                    out.fill = tok[5];
                    continue;
                }

                // .N  → précision
                if (tok[0] == '.') {
                    const char* n = tok + 1;
                    out.precision = ParseInt(&n);
                    continue;
                }

                // Nombre seul → width implicite
                if (tok[0] >= '1' && tok[0] <= '9') {
                    const char* n = tok;
                    out.width = ParseInt(&n);
                    continue;
                }
            }

            return out;
        }

        // =====================================================================
        //  NkApplyFormatProps
        // =====================================================================

        NkString NkApplyFormatProps(const NkString& value, const NkFormatProps& props) {
            if (!props.HasWidth()) return value;

            int vlen = static_cast<int>(value.Length());
            int w    = props.width;
            if (vlen >= w) return value;

            int pad   = w - vlen;
            NkFormatAlign align = props.align;
            if (align == NkFormatAlign::Unset) align = NkFormatAlign::Right;

            NkString result;
            result.Reserve(static_cast<NkString::SizeType>(w));

            switch (align) {
                case NkFormatAlign::Left: {
                    result.Append(value);
                    result.Append(static_cast<NkString::SizeType>(pad), props.fill);
                    break;
                }
                case NkFormatAlign::Right: {
                    result.Append(static_cast<NkString::SizeType>(pad), props.fill);
                    result.Append(value);
                    break;
                }
                case NkFormatAlign::Center: {
                    int lp = pad / 2, rp = pad - lp;
                    result.Append(static_cast<NkString::SizeType>(lp), props.fill);
                    result.Append(value);
                    result.Append(static_cast<NkString::SizeType>(rp), props.fill);
                    break;
                }
                default:
                    result.Append(value);
                    break;
            }
            return result;
        }

        // =====================================================================
        //  NkIntToString
        // =====================================================================

        NkString NkIntToString(long long v, const NkFormatProps& props) {
            char buf[128];
            bool negative = (v < 0);
            unsigned long long uv = negative
                ? static_cast<unsigned long long>(-v)
                : static_cast<unsigned long long>(v);

            switch (props.base) {
                case NkFormatBase::Bin: {
                    NkString s;
                    if (negative)         s.Append('-');
                    else if (props.showSign) s.Append('+');
                    if (props.showPrefix) s.Append("0b");
                    s.Append(NkToBinaryString(uv, 0, '0'));
                    return NkApplyFormatProps(s, props);
                }
                case NkFormatBase::Oct: {
                    ::snprintf(buf, sizeof(buf), props.showPrefix ? "0%llo" : "%llo", uv);
                    NkString s;
                    if (negative)            s.Append('-');
                    else if (props.showSign) s.Append('+');
                    s.Append(buf);
                    return NkApplyFormatProps(s, props);
                }
                case NkFormatBase::Hex: {
                    ::snprintf(buf, sizeof(buf), "%llx", uv);
                    NkString s;
                    if (negative)            s.Append('-');
                    else if (props.showSign) s.Append('+');
                    if (props.showPrefix)    s.Append("0x");
                    s.Append(buf);
                    return NkApplyFormatProps(s, props);
                }
                case NkFormatBase::HexUpper: {
                    ::snprintf(buf, sizeof(buf), "%llX", uv);
                    NkString s;
                    if (negative)            s.Append('-');
                    else if (props.showSign) s.Append('+');
                    if (props.showPrefix)    s.Append("0X");
                    s.Append(buf);
                    return NkApplyFormatProps(s, props);
                }
                default: break;
            }

            // Décimal
            ::snprintf(buf, sizeof(buf), "%lld", v);
            NkString s(buf);
            if (!negative && props.showSign && v >= 0) s.Insert(0u, "+");
            return NkApplyFormatProps(s, props);
        }

        // =====================================================================
        //  NkUIntToString
        // =====================================================================

        NkString NkUIntToString(unsigned long long v, const NkFormatProps& props) {
            char buf[128];

            switch (props.base) {
                case NkFormatBase::Bin: {
                    NkString s;
                    if (props.showSign)   s.Append('+');
                    if (props.showPrefix) s.Append("0b");
                    s.Append(NkToBinaryString(v, 0, '0'));
                    return NkApplyFormatProps(s, props);
                }
                case NkFormatBase::Oct: {
                    ::snprintf(buf, sizeof(buf), props.showPrefix ? "0%llo" : "%llo", v);
                    NkString s;
                    if (props.showSign) s.Append('+');
                    s.Append(buf);
                    return NkApplyFormatProps(s, props);
                }
                case NkFormatBase::Hex: {
                    ::snprintf(buf, sizeof(buf), "%llx", v);
                    NkString s;
                    if (props.showSign)   s.Append('+');
                    if (props.showPrefix) s.Append("0x");
                    s.Append(buf);
                    return NkApplyFormatProps(s, props);
                }
                case NkFormatBase::HexUpper: {
                    ::snprintf(buf, sizeof(buf), "%llX", v);
                    NkString s;
                    if (props.showSign)   s.Append('+');
                    if (props.showPrefix) s.Append("0X");
                    s.Append(buf);
                    return NkApplyFormatProps(s, props);
                }
                default: break;
            }

            ::snprintf(buf, sizeof(buf), "%llu", v);
            NkString s(buf);
            if (props.showSign) s.Insert(0u, "+");
            return NkApplyFormatProps(s, props);
        }

        // =====================================================================
        //  NkFloatToString
        // =====================================================================

        NkString NkFloatToString(double v, const NkFormatProps& props) {
            char fmt[32], buf[128];

            char signFlag = props.showSign ? '+' : '\0';
            char spec = 'f';
            switch (props.floatStyle) {
                case NkFormatFloat::Scientific: spec = 'e'; break;
                case NkFormatFloat::SciUpper:   spec = 'E'; break;
                case NkFormatFloat::General:    spec = 'g'; break;
                default:                        spec = 'f'; break;
            }

            if (props.HasPrecision()) {
                if (signFlag)
                    ::snprintf(fmt, sizeof(fmt), "%%%c.%d%c", signFlag, props.precision, spec);
                else
                    ::snprintf(fmt, sizeof(fmt), "%%.%d%c", props.precision, spec);
            } else {
                if (signFlag)
                    ::snprintf(fmt, sizeof(fmt), "%%%c%c", signFlag, spec);
                else
                    ::snprintf(fmt, sizeof(fmt), "%%%c", spec);
            }

            ::snprintf(buf, sizeof(buf), fmt, v);
            return NkApplyFormatProps(NkString(buf), props);
        }

        // =====================================================================
        //  NkFormatImpl — moteur principal
        // =====================================================================

        NkString NkFormatImpl(const char* fmt, const NkFormatArg* args, int argCount) {
            NkString result;
            if (!fmt) return result;

            result.Reserve(static_cast<NkString::SizeType>(::strlen(fmt) * 2));

            const char* p = fmt;

            while (*p) {
                // '{{' → '{'
                if (p[0] == '{' && p[1] == '{') { result.Append('{'); p += 2; continue; }
                // '}}' → '}'
                if (p[0] == '}' && p[1] == '}') { result.Append('}'); p += 2; continue; }

                // Placeholder '{i}' ou '{i:props}'
                if (*p == '{') {
                    ++p;

                    // Parse index
                    int idx = 0;
                    while (*p >= '0' && *p <= '9') {
                        idx = idx * 10 + (*p - '0');
                        ++p;
                    }

                    // Props optionnelles
                    const char* propsStart = nullptr;
                    int         propsLen   = 0;
                    if (*p == ':') {
                        ++p;
                        propsStart = p;
                        while (*p && *p != '}') ++p;
                        propsLen = static_cast<int>(p - propsStart);
                    }

                    if (*p == '}') ++p;

                    // Index invalide ou pas d'args
                    if (!args || idx < 0 || idx >= argCount) {
                        result.Append("{?}");
                        continue;
                    }

                    NkFormatProps props = NkParseFormatProps(propsStart, propsLen);
                    result.Append(args[idx].convert(args[idx].data, props));
                    continue;
                }

                // Texte littéral
                result.Append(*p++);
            }

            return result;
        }

    
} // namespace nkentseu